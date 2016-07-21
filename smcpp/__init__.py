# Apply a monkeypatch to ad to prevent the O(k^2) computation of 2nd derivatives
import logging
import ad

def init_logging():
    # Get rid of any pre-existing stuff
    root = logging.getLogger()
    while len(root.handlers) > 0:
        root.removeHandler(logging.root.handlers[-1])
    logging.addLevelName(logging.DEBUG - 1, 'DEBUG1')
    fmt = logging.Formatter('%(relativeCreated)d %(name)-12s %(levelname)-1s %(message)s')
    sh = logging.StreamHandler()
    sh.setFormatter(fmt)
    sh.setLevel(logging.INFO)
    root.addHandler(sh)
    root.setLevel(logging.NOTSET)
    logging.getLogger('momi').setLevel(logging.INFO)

init_logging()

def _my_apply_chain_rule(ad_funcs, variables, lc_wrt_args, qc_wrt_args, cp_wrt_args):
    "Monkey-patched version of ad._apply_chain_rule which only computes first derivatives"
    lc_wrt_vars = dict((var, 0.) for var in variables)
    for j, var1 in enumerate(variables):
        for (f, dh) in zip(ad_funcs, lc_wrt_args):
            fdv1 = f.d(var1)
            # first order terms
            lc_wrt_vars[var1] += dh*fdv1
    return (lc_wrt_vars, {}, {})
ad._apply_chain_rule = _my_apply_chain_rule
